#include <complex>
#include "expression.h"
#include <QWidget>
#include "ui_expressiondialog.h"
#include <QWidget>
#include <QDebug>
#include "Traces/trace.h"
#include "ui_expressionexplanationwidget.h"

using namespace mup;
using namespace std;

Math::Expression::Expression()
{
    parser = new ParserX(pckCOMMON | pckUNIT | pckCOMPLEX);
    parser->DefineVar("x", Variable(&x));
    parser->DefineVar("z", Variable(&z));
    expressionChanged();
}

Math::Expression::~Expression()
{
    delete parser;
}

TraceMath::DataType Math::Expression::outputType(TraceMath::DataType inputType)
{
    return inputType;
}

QString Math::Expression::description()
{
    return "Custom expression: " + exp;
}

void Math::Expression::edit()
{
    auto d = new QDialog();
    auto ui = new Ui::ExpressionDialog;
    ui->setupUi(d);
    ui->expEdit->setText(exp);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, [=](){
        exp = ui->expEdit->text();
        expressionChanged();
    });
    if(dataType == DataType::Time) {
        // select the label explaining the time domain variables (frequency label is the default)
        ui->stackedWidget->setCurrentIndex(1);
    }
    d->show();
}

QWidget *Math::Expression::createExplanationWidget()
{
    auto w = new QWidget();
    auto ui = new Ui::ExpressionExplanationWidget;
    ui->setupUi(w);
    return w;
}

nlohmann::json Math::Expression::toJSON()
{
    nlohmann::json j;
    j["exp"] = exp.toStdString();
    return j;
}

void Math::Expression::fromJSON(nlohmann::json j)
{
    exp = QString::fromStdString(j.value("exp", ""));
    expressionChanged();
}

//Normalize to [-180,180):
inline double constrainAngle(double x){
    x = fmod(x + M_PI, M_PI * 2);
    if (x < 0)
        x += M_PI * 2;
    return x - M_PI;
}
// convert to [-360,360]
inline double angleConv(double angle){
    return fmod(constrainAngle(angle), M_PI * 2);
}
inline double angleDiff(double a,double b){
    double dif = fmod(b - a + M_PI, M_PI * 2);
    if (dif < 0)
        dif += M_PI * 2;
    return dif - M_PI;
}
inline double unwrap(double previousAngle,double newAngle){
    return previousAngle - angleDiff(newAngle,angleConv(previousAngle));
}

void Math::Expression::inputSamplesChanged(unsigned int begin, unsigned int end)
{
    auto in = input->rData();
    data.resize(in.size());
    try {
        double lastAng;
        for(unsigned int i=begin;i<end;i++) {
            t = in[i].x;
            f = in[i].x;
            w = in[i].x * 2 * M_PI;
            d = root()->timeToDistance(t);
            x = in[i].y;

            double Z0 = 50;
	        double MAG = abs(complex<double>(x));
            double ANG = arg(complex<double>(x)) * 180 / M_PI;
            if(i == begin) lastAng = ANG;
            ANG = unwrap(lastAng, ANG);
            const double real = (Z0*(1-(MAG*MAG)))/(1+(MAG*MAG)-(2*MAG*cos((ANG / 360)*2*M_PI)));
            const double imag = (2*MAG*sin((ANG / 360)*2*M_PI)*Z0)/(1+(MAG*MAG)-(2*MAG*cos((ANG / 360)*2*M_PI)));
            z = real + imag * 1.0i;
            lastAng = ANG;

            Value res = parser->Eval();
            data[i].x = in[i].x;
            data[i].y = res.GetComplex();
        }
        success();
        emit outputSamplesChanged(begin, end);
    } catch (const ParserError &e) {
        error(QString::fromStdString(e.GetMsg()));
    }
}

void Math::Expression::expressionChanged()
{
    if(exp.isEmpty()) {
        error("Empty expression");
        return;
    }
    parser->SetExpr(exp.toStdString());
    parser->RemoveVar("t");
    parser->RemoveVar("d");
    parser->RemoveVar("f");
    parser->RemoveVar("w");
    switch(dataType) {
    case DataType::Time:
        parser->DefineVar("t", Variable(&t));
        parser->DefineVar("d", Variable(&d));
        break;
    case DataType::Frequency:
        parser->DefineVar("f", Variable(&f));
        parser->DefineVar("w", Variable(&w));
        break;
    default:
        break;
    }
    if(input) {
        inputSamplesChanged(0, input->rData().size());
    }
}
